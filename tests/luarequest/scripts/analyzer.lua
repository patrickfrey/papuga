
function PUT( context, input)
end

function GET( context, input)
	query = schema( "query", input)
	--for fidx,feat in ipairs(query.feature) do
	--	feat.analyzed = {type = feat.content.type, value = feat.content.value:lower()}
	--end
	print "HALLY GALLY !!!\n"
	return query
end

