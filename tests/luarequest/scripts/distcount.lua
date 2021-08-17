function PUT( context, input)
	server = schema( "query", input ).query
	context:set( "tokenizer", "T")
	context:set( "normalizer", "N")
end

function GET( context, input)
	
	query = schema( "query", input ).query

	for fidx,feat in ipairs(query.feature) do
		feat.analyzed = {type = feat.content.type, value = feat.content.value:lower()}
	end
	return {query=query}
end

