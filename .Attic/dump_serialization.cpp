
static void dump( const papuga_Serialization& ser)
{
	papuga_SerializationIter sitr;
	papuga_init_SerializationIter( &sitr, &ser);
	while (!papuga_SerializationIter_eof( &sitr))
	{
		const papuga_ValueVariant* val = papuga_SerializationIter_value( &sitr);
		const char* valstr = "";
		if (val && val->valuetype == papuga_TypeString) valstr = val->value.string;
		std::cerr << papuga_Tag_name( papuga_SerializationIter_tag( &sitr)) << " " << valstr << std::endl;
		papuga_SerializationIter_skip( &sitr);
	}
}

