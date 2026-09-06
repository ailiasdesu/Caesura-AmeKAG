// Wasmoon string proxies terminate at NUL. Escape values in Lua before they
// cross into JS, retaining JSON's ordinary on-disk format and full UTF-8 text.
export async function installSaveValueBridge(lua, saveGame) {
  lua.global.set('__SAVE_JSON_VALUE', json => saveGame(...JSON.parse(json)))
  await lua.doString(String.raw`
    local write = __SAVE_JSON_VALUE
    local function quoted(value)
      if not utf8.len(value) then error('Invalid UTF-8 save value',0) end
      return '"'..value:gsub('[%z\1-\31\\"]', function(ch)
        return string.format('\\u%04x', string.byte(ch))
      end)..'"'
    end
    local function encode(value, depth)
      if depth > 64 then error('Save value depth limit',0) end
      local kind=type(value)
      if kind=='string' then return quoted(value) end
      if kind=='boolean' then return tostring(value) end
      if kind=='number' and value==value and math.abs(value)~=math.huge then
        if value%1==0 and (value < -9007199254740991 or value > 9007199254740991) then error('Save integer exceeds Web JSON safe range',0) end
        return tostring(value)
      end
      if kind~='table' then error('Unsupported save JSON value',0) end
      local items={}
      if #value>0 then
        local count=0
        for key in pairs(value) do
          if type(key)~='number' or key%1~=0 or key<1 or key>#value then
            error('Save array must be dense and contain only integer keys',0)
          end
          count=count+1
        end
        if count~=#value then error('Save array contains holes',0) end
        for i=1,#value do items[i]=encode(value[i],depth+1) end
        return '['..table.concat(items,',')..']'
      end
      for key,item in pairs(value) do
        if type(key)~='string' then error('Unsupported save JSON key',0) end
        items[#items+1]=quoted(key)..':'..encode(item,depth+1)
      end
      return '{'..table.concat(items,',')..'}'
    end
    KAG.save_game=function(slot,state,scene,index,thumbnail)
      local ok, json=pcall(encode,{slot,state,scene,index,thumbnail or ''},0)
      if not ok then return false end
      return write(json)
    end
    __SAVE_JSON_VALUE=nil
  `)
}
