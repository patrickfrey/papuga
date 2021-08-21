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

function GET( context, input)
	query = schema( "query", input ).query

	for fidx,feat in ipairs(query.feature) do
		feat.analyzed = {type = feat.content.type, value = feat.content.value:lower()}
	end
	return {query=query}
end
