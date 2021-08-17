function GET( context, input)
	person = schema( "person", input ).person
	return {count=person.age}
end

