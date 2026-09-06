*start
[set var="lf.owner" value="caller"]
[call target="*sub"]
[eval exp="f.returned_owner = lf.owner"]
[set var="f.finished" value=true]
[end]
*sub
[set var="lf.owner" value="callee"]
[set var="f.saved_value" value=42]
[save slot=34]
