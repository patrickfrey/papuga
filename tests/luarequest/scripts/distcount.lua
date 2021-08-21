local function dump( st)
	if type( st) == "table" then
		local content = nil
		if st[ #st] then
			for _,vv in ipairs( st) do
				content = content
					and (content .. ", " .. dump(vv))
					or dump(vv)
			end
		else
			for kk,vv in pairs( st) do
				content = content 
					and (content .. ", " .. kk .. "=" .. dump(vv))
					or (kk .. "=" .. dump(vv))
			end
		end
		return "{" .. (content and content or "") .. "}"
	elseif st then
		return "'" .. tostring( st) .. "'"
	else
		return "nil"
	end
end

function PUT( context, input)
	local servers = schema( "distcount", input ).distcount.server
	local responselist = {}
	context:set( "server", servers)
	for _,server in ipairs(servers) do
		table.insert( responselist, send( "PUT", server.address, {count_config=server}))
	end
	yield()
	for ri,response in ipairs(responselist) do
		local errcode,errmsg = response:error()
		if errcode then
			error( "server " .. ri .. " error: " .. errcode .. ": " .. errmsg )
		end
	end
end

function GET( context, input)
	local servers = context:get( "server")
	local count = 0
	local responselist = {}
	local doctype,encoding = content( input)

	for _,server in ipairs(servers) do
		table.insert( responselist, send( "GET", server.address, input))
	end
	yield()
	for ri,response in ipairs(responselist) do
		local errcode,errmsg = response:error()
		if errcode then
			error( "server " .. ri .. " error: " .. errcode .. ": " .. errmsg )
		end
		local result = response:result()
		count = count + result.count 
	end
	return {count=count}
end
