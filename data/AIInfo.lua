--
--  Info Definition Table format
--
--
--  These keywords must be lowercase for LuaParser to read them.
--
--  key:      user defined or one of the SKIRMISH_AI_PROPERTY_* defines in
--            SSkirmishAILibrary.h
--  value:    the value of the property
--  desc:     the description (could be used as a tooltip)
--
--
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local infos = {
	{
		key    = 'shortName',
		value  = 'BaczekKPAI',
		desc   = 'machine conform name.',
	},
	{
		key    = 'version',
		value  = '1.2', -- AI version - !This comment is used for parsing!
	},
	{
		key    = 'name',
		value  = 'Baczek\'s KP AI',
		desc   = 'human readable name.',
	},
	{
		key    = 'description',
		value  = 'AI for Kernel Panic. Plays System well, Hacker so-so, Network rather bad.',
		desc   = 'tooltip.',
	},
	{
		key    = 'url',
		value  = 'http://spring.clan-sy.com/wiki/AI:BaczekKPAI',
		desc   = 'URL with more detailed info about the AI',
	},
	{
		key    = 'loadSupported',
		value  = 'no',
		desc   = 'whether this AI supports loading or not',
	},
	{
		key    = 'interfaceShortName',
		value  = 'C', -- AI Interface name - !This comment is used for parsing!
		desc   = 'the shortName of the AI interface this AI needs',
	},
	{
		key    = 'interfaceVersion',
		value  = '0.1', -- AI Interface version - !This comment is used for parsing!
		desc   = 'the minimum version of the AI interface this AI needs',
	},
}

return infos
