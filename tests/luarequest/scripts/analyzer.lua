function PUT( context, input)
	context:set( "tokenizer", "T")
	context:set( "normalizer", "N")
end

local function keys( stu)
	local rt = {}
	if type(stu) == "table" then
		for key,_ in pairs(stu) do
			table.insert( rt, key)
		end
	end
	return rt
end

local function dump_( stu, indent)
	if type(stu) == "table" then
		for key,val in pairs(stu) do
			io.stderr:write( indent .. "'" .. key .. "' :")
			dump_( val, indent .. "  ")
		end
	else
		io.stderr:write( " '" .. stu .. "'")
	end
end
local function dump( stu)
	dump_( stu, "\n")
	io.stderr:write( "\n")
end

function GET( context, input)
	query = schema( "query", input ).query

	for fidx,feat in ipairs(query.feature) do
		feat.analyzed = {type = feat.content.type, value = feat.content.value:lower()}
	end
	return {query=query}
end
