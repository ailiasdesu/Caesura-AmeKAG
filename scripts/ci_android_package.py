#!/usr/bin/env python3
"""Hosted Android package gate using existing JNI and shared TEST-signing checks.

This lane resolves Gradle dependencies online. It does not claim the full
run_android_validation offline build provenance or device execution.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, sys
from pathlib import Path
import run_android_validation as driver
import android_package_contract as package
from ci_package_lane import _new_work, _outputs
from verify_execution_bundle import _no_links

SCHEMA = 'caesura.ci-android-package.v1'
BUNDLETOOL_URL = 'https://github.com/google/bundletool/releases/download/1.17.1/bundletool-all-1.17.1.jar'
# Official release asset 181329951, downloaded 2026-09-30 over HTTPS. The old
# GitHub asset has no publisher digest; this pins those observed original bytes.
BUNDLETOOL_SHA256 = '45881ead13388872d82c4255b195488b7fc33f2cac5a9a977b0afc5e92367592'
need, lock, save = driver.need, driver.lock, driver.save


def verify_bundletool(path):
    selected = lock(Path(path).resolve(strict=True))
    need(selected['sha256'] == BUNDLETOOL_SHA256, 'Unpinned bundletool bytes')
    return selected


def _component(root, paths):
    root = Path(root).resolve(strict=True)
    return dict(root=str(root), paths=paths, **driver._tree(root, paths))


def _select_tools(jdk, sdk, ndk, gradle, sdl, bundletool, *, fixture):
    components = dict(jdk=_component(jdk, ['bin', 'lib', 'conf', 'release']),
        sdk=_component(sdk, ['build-tools/34.0.0', 'platforms/android-35']),
        ndk=_component(ndk, ['source.properties']), gradle=_component(gradle, ['lib']),
        sdl=_component(sdl, ['lib/libSDL3.so']))
    need(re.search(r'^JAVA_VERSION="17(?:\.|\")', (Path(components['jdk']['root'])/'release').read_text(), re.M), 'JDK17 required')
    for role, rel, pattern in [('ndk','source.properties',r'Pkg.Revision\s*=\s*27\.3\.13750724'),
            ('sdk','build-tools/34.0.0/source.properties',r'Pkg.Revision\s*=\s*34\.0\.0')]:
        need(re.search(pattern, (Path(components[role]['root'])/rel).read_text()), 'Wrong selected '+role)
    suffix = '.exe' if os.name == 'nt' and not fixture else ''
    jdk = Path(components['jdk']['root']); bt = Path(components['sdk']['root'])/'build-tools/34.0.0'
    tools = {name:lock(jdk/'bin'/(name+suffix)) for name in ('java','keytool','jarsigner')}
    tools.update({name:lock(bt/(name+suffix)) for name in ('aapt2','zipalign')})
    tools['apksigner_jar'] = lock(bt/'lib/apksigner.jar')
    tools['bundletool_jar'] = lock(bundletool) if fixture else verify_bundletool(bundletool)
    git = shutil.which('git'); need(git, 'Git required')
    tools['git'] = lock(Path(git).resolve(strict=True))
    return dict(components=components, tools=tools)


def _stable(report):
    for selected in [report['native']['library'], *report['outputs'].values(), *report['toolchain']['tools'].values()]:
        package._file(selected['path'], selected['sha256'])
    for component in report['toolchain']['components'].values():
        need(driver._tree(Path(component['root']), component['paths']) == {k:component[k] for k in ('files','directories')}, 'Selected runtime dependency changed')
    need(driver._source(Path(report['repo']), report['toolchain']['tools']['git']['path'],
                        driver._env(report['toolchain'], Path(report['work']))) == report['source'], 'Source changed')
    need(driver._stage_snapshot(Path(report['staging']['path'])) == report['staging']['snapshot'], 'Staging changed')
    for item in report['unsigned'].values():
        for key in ('file','producer_file'):package._file(item[key]['path'],item[key]['sha256'])
        driver.verify_stable(item['prepared'])
    for command in report['commands']:
        for selected in [command['stdout'],command['stderr'],*command['process_files']]:
            package._file(selected['path'],selected['sha256'])
    package.verify_android_package_stable(report['package'])
    need(driver._signature_closure(report) == report['signature_transform'], 'Signature closure changed')
    need(report['private_cleanup']=='COMPLETE' and not (Path(report['work'])/'private-signing').exists(), 'Private signing cleanup incomplete')


def verify_ci_android_package_stable(result):
    need(result.get('schema')==SCHEMA and result.get('status') in ('CI_ANDROID_PACKAGE_VERIFIED','FIXTURE_ONLY')
         and result.get('release_ready') is False, 'Completed CI Android package result required')
    saved=driver.load(result['receipt_path'],result['receipt_sha256'])
    need(saved=={k:v for k,v in result.items() if k!='receipt_sha256'}, 'CI receipt differs')
    _stable(result)
    work=Path(result['work']);proof=work/'proof'
    expected=[]
    for path in _proof_sources(result):
        record=lock(path);rel=path.relative_to(work)
        package._file(proof/rel,record['sha256'])
        expected.append(dict(file=rel.as_posix(),sha256=record['sha256']))
    actual=json.loads((proof/'inventory.json').read_text(encoding='utf-8'))
    need(actual==dict(files=expected,scope='Original small evidence; final APK/AAB bytes are separate artifacts'),'Proof inventory differs')
    need(set(driver._tree(proof,['.'])['files'])=={x['file'] for x in expected}|{'inventory.json'},'Proof has unexpected files')
    return dict(status='CI_ANDROID_PACKAGE_STABLE',release_ready=False)


def _proof_sources(report):
    work=Path(report['work'])
    selected=[Path(report['receipt_path'])]
    for command in report.get('commands',[]):
        selected.extend(Path(command[k]['path']) for k in ('stdout','stderr'))
        selected.extend(Path(v['path']) for v in command['process_files'])
    check=report.get('package')
    if check:
        selected.extend([Path(check['receipt_path']),Path(check['control']['path'])])
        for command in check['commands']:
            selected.extend(Path(command[k]['path']) for k in ('stdout','stderr'))
            selected.extend(Path(v['path']) for v in command['process_files'])
    elif (work/'verify/android-package.json').is_file():
        selected.append(work/'verify/android-package.json')
        # A failed verifier still owns its tool diagnostics.
        selected.extend((work/'verify').glob('*.stdout'))
        selected.extend((work/'verify').glob('*.stderr'))
        selected.extend((work/'verify').glob('*-process/*.json'))
    certificate=work/'outputs/test-certificate.der'
    if certificate.is_file():selected.append(certificate)
    return sorted(set(selected))


def _proof(report):
    """Copy original small receipts/logs, never keys or expanded archive payloads."""
    work=Path(report['work']); proof=work/'proof'; proof.mkdir()
    inventory=[]
    for path in _proof_sources(report):
        need(path.is_relative_to(work) and 'private-signing' not in path.parts, 'Unsafe proof selection')
        record=lock(path);rel=path.relative_to(work);target=proof/rel
        driver._copy(path,target,record['sha256']);inventory.append(dict(file=rel.as_posix(),sha256=record['sha256']))
    save(proof/'inventory.json',dict(files=inventory,scope='Original small evidence; final APK/AAB bytes are separate artifacts'))


def run_ci_android_package(*,repo,source_sha,native_library,sdl_root,jdk_root,sdk_root,ndk_root,
                           gradle_root,bundletool_jar,work_dir,runner=None):
    work=_new_work(work_dir); receipt=work/'ci-android-package.json'
    report=dict(schema=SCHEMA,status='FAIL',release_ready=False,device='NOT_RUN',runtime='NOT_RUN',install='NOT_RUN',
        work=str(work),repo=str(Path(repo).resolve(strict=True)),receipt_path=str(receipt),commands=[],errors=[],
        private_cleanup='NOT_CREATED',dependency_scope='HOSTED_ONLINE_GRADLE_NOT_OFFLINE_DRIVER',
        native_provenance='EXISTING_JNI_BYTES_NOT_REBUILT_BY_THIS_CONTROLLER',
        producer_provenance='CALLER_MUST_AUTHENTICATE_JOB_SOURCE_RUN_ATTEMPT',
        limits=['Selected JDK/SDK/Gradle files inventoried, not a hermetic host',
                'NDK source.properties checked; this controller does not attest the prior compiler process',
                'AAB semantics cover base identity/version/SDK only; signatures use an ephemeral TEST key'])
    try:
        for name in ('home','tmp','commands','gradle-home','unsigned','aligned','outputs'):(work/name).mkdir()
        need(re.fullmatch('[0-9a-f]{40}',source_sha or ''), 'Full source SHA required')
        tc=_select_tools(jdk_root,sdk_root,ndk_root,gradle_root,sdl_root,bundletool_jar,fixture=runner is not None)
        report['toolchain']=tc;env=driver._env(tc,work)
        source=driver._source(Path(report['repo']),tc['tools']['git']['path'],env)
        need(source['source_sha']==source_sha,'Source HEAD differs')
        report['source']=source
        versions=re.findall(r'project\s*\(\s*CaesuraAmeKAG\s+VERSION\s+(\d+\.\d+\.\d+)',
                            (Path(report['repo'])/'CMakeLists.txt').read_text(encoding='utf-8'),re.I)
        need(len(versions)==1,'Exactly one controlled engine version required')
        value=dict(repo=report['repo'],source_sha=source_sha,version_name=versions[0],version_code=1,
            package_name='com.caesura.app',abi='arm64-v8a',min_sdk=24,target_sdk=35,
            game_relative_path='tests/projects/first_vn',jobs=3,timeouts=dict(gradle=900,sign=120,verify=120))
        report['inputs']=value
        library=lock(native_library);package._elf(Path(library['path']));report['native']=dict(library=library)
        report['staging']=driver._stage(value,source,report['native'],tc,dict(files={}),work)
        command=driver.Commands(report,work,env,runner)
        text=command.run('java-version',[tc['tools']['java']['path'],'-version'],60)
        need(re.search(r'\bversion "17(?:\.|\")',text),'Actual Java is not JDK17')
        report['unsigned']=driver._gradle(value,tc,report['staging'],work,command,offline=False,include_debug=True)
        report['outputs']=driver._sign(value,tc,report['unsigned'],command,work,report)
        expected={k:value[k] for k in ('source_sha','package_name','version_name','version_code','abi','min_sdk','target_sdk')}
        expected.update(certificate_sha256=report['outputs']['certificate']['sha256'],
            required_apk_entries=report['unsigned']['apk']['entries'],required_aab_entries=report['unsigned']['aab']['entries'])
        report['package']=package.verify_android_package(apk_path=report['outputs']['apk']['path'],
            apk_sha256=report['outputs']['apk']['sha256'],aab_path=report['outputs']['aab']['path'],
            aab_sha256=report['outputs']['aab']['sha256'],expected=expected,
            tools={k:tc['tools'][k] for k in package.TOOL_NAMES},work_dir=work/'verify',runner=runner,timeout=120)
        report['signature_transform']=driver._signature_closure(report)
        driver._cleanup_private(work,report);_stable(report)
        report['status']='FIXTURE_ONLY' if runner is not None else 'CI_ANDROID_PACKAGE_VERIFIED'
    except Exception as error:
        report['errors'].append(str(error));raise
    finally:
        try:
            if (work/'private-signing').exists() and report['private_cleanup']!='FAILED':driver._cleanup_private(work,report)
        except Exception as error:
            report.update(status='FAIL',private_cleanup='FAILED');report['errors'].append('Private cleanup: '+str(error))
        save(receipt,report);_proof(report)
    need(report['status']!='FAIL','Private cleanup failed')
    return dict(report,receipt_sha256=package._sha256_file(receipt))


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='mode',required=True)
    run=sub.add_parser('run')
    for name in ('repo','source-sha','native-library','sdl-root','jdk-root','sdk-root','ndk-root','gradle-root','bundletool-jar','work'):
        run.add_argument('--'+name,required=True)
    run.add_argument('--github-output')
    verify=sub.add_parser('verify');verify.add_argument('--receipt',required=True);verify.add_argument('--sha256',required=True)
    args=parser.parse_args(argv)
    try:
        if args.mode=='verify':
            result=driver.load(args.receipt,args.sha256);result['receipt_sha256']=args.sha256
            need(result.get('status')=='CI_ANDROID_PACKAGE_VERIFIED','Real CI package evidence required')
            print(json.dumps(verify_ci_android_package_stable(result)));return 0
        values=vars(args).copy();values.pop('mode');output=values.pop('github_output');values['work_dir']=values.pop('work')
        result=run_ci_android_package(**values)
        verify_ci_android_package_stable(result)
        _outputs(Path(output) if output else None,dict(receipt=result['receipt_path'],receipt_sha256=result['receipt_sha256'],
            upload_files='\n'.join(result['outputs'][kind]['path'] for kind in ('apk','aab')),proof=str(Path(result['work'])/'proof')))
        print(json.dumps({k:result[k] for k in ('status','receipt_path','receipt_sha256','release_ready')}));return 0
    except Exception as error:
        print(json.dumps(dict(status='FAIL',release_ready=False,error=str(error))));return 1

if __name__=='__main__':
    sys.dont_write_bytecode=True
    raise SystemExit(main())