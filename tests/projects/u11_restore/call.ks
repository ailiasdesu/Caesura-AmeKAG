*start
[set var="f.visits" value=0]
[set var="lf.owner" value="caller"]
[for var="i" start=1 end=2 step=1]
[if exp="f.i"]
[call target="*sub"]
[endif]
[endfor]
[eval exp="f.caller_owner = lf.owner"]
[set var="f.finished" value=true]
[end]
*sub
[set var="lf.owner" value="callee"]
[jump target="*inside"]
*inside
[wait time=60000]
[eval exp="f.callee_owner = lf.owner"]
[eval exp="f.visits = f.visits + 1"]
[return]
