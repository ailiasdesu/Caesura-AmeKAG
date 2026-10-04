// Transfer common renderer pages in one bounded value instead of thousands of
// Wasmoon table-field crossings. Unsupported shapes retain the original reader.
// ASCII-only eligibility preserves the old Wasmoon UTF-8, BOM and NUL semantics.
export async function createBacklogReader(lua) {
  const encode = await lua.doString(String.raw`
-- One bounded reader for the renderer-produced ASCII {t,x,y} page shape.
local globals=_G
local rawget,next,type,getmetatable=rawget,next,type,getmetatable
local find,gsub,format=string.find,string.gsub,string.format
local concat,huge=table.concat,math.huge
local MAX_BYTES,MAX_STRING_BYTES,MAX_NODES=1048576,262144,65536
local escapes={['"']='\\"',['\\']='\\\\'}
for byte=0,31 do escapes[string.char(byte)]=format('\\u%04x',byte) end
local function dense(value)
 if type(value)~='table' or getmetatable(value)~=nil then return nil end
 local count,maximum=0,0
 for key in next,value do
  if type(key)~='number' or key~=key or key%1~=0 or key<1 or key>MAX_NODES then return nil end
  count=count+1;if key>maximum then maximum=key end
 end
 if maximum~=count then return nil end
 return count
end
local function valid(n) return type(n)=='number' and n==n and n~=huge and n~=-huge and n>=-9007199254740991 and n<=9007199254740991 end
return function()
 local pages=rawget(globals,'__SCENE_BACKLOG')
 if pages==nil then return nil end
 local pageCount=dense(pages)
 if not pageCount then return nil end
 if pageCount==0 then return '{}' end
 local parts,bytes,nodes={'['},1,1
 local function append(value)
  bytes=bytes+#value
  if bytes>MAX_BYTES then return false end
  parts[#parts+1]=value;return true
 end
 for pi=1,pageCount do
  local page=rawget(pages,pi)
  local rows=dense(page)
  if not rows then return nil end
  nodes=nodes+1
  if nodes>MAX_NODES then return nil end
  if pi>1 and not append(',') then return nil end
  if not append(rows==0 and '{}' or '[') then return nil end
  for ri=1,rows do
   local row=rawget(page,ri)
   if type(row)~='table' or getmetatable(row)~=nil then return nil end
   local count=0
   for key in next,row do
    if key~='t' and key~='x' and key~='y' then return nil end
    count=count+1
   end
   if count~=3 then return nil end
   local t,x,y=rawget(row,'t'),rawget(row,'x'),rawget(row,'y')
   if type(t)~='string' or #t>MAX_STRING_BYTES then return nil end
   -- cwrap("...", "string") uses NUL-terminated UTF-8. A NUL-bearing
   -- original field would be truncated by old get; do not change that path.
   -- Restrict to ASCII so Wasmoon UTF-8/BOM/invalid-byte interpretation
   -- remains exclusively on the existing path for every non-ASCII string.
   if find(t,'%z') or find(t,'[\128-\255]') then return nil end
   if not valid(x) or not valid(y) then return nil end
   nodes=nodes+4
   if nodes>MAX_NODES then return nil end
   if ri>1 and not append(',') then return nil end
   local escaped=gsub(t,'[%z\1-\31\\"]',escapes)
   local numberX=x==0 and '0' or gsub(format('%.17g',x),',','.')
   local numberY=y==0 and '0' or gsub(format('%.17g',y),',','.')
   if not append('{"t":"'..escaped..'","x":'..numberX..',"y":'..numberY..'}') then return nil end
  end
  if rows>0 and not append(']') then return nil end
 end
 if not append(']') then return nil end
 return concat(parts)
end
`)
  if (typeof encode !== 'function') throw new TypeError('Backlog encoder did not return a function')
  return () => {
    const text = encode()
    if (text === null) return {pages:lua.global.get('__SCENE_BACKLOG'),encoded:false}
    if (typeof text !== 'string') throw new TypeError('Backlog encoder returned an invalid value')
    // Unexpected encoder or parser failures propagate; fallback is only an
    // explicit eligibility refusal, never an error-masking recovery path.
    return {pages:JSON.parse(text),encoded:true}
  }
}
