function PUT( context, input)
	config = schema( "count_config", input ).count_config
	context:set( "config", config)
end

function GET( context, input)
	record = schema( "count", input ).count
	config = context:get( "config")
	local count = record.value * config.factor
	return {count=count}
end

