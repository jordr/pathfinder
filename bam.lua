
-- Settings
settings = NewSettings()

-- Wall option
if true or ScriptArgs["wall"]=="true" or ScriptArgs["Wall"]=="true" then
	settings.cc.flags:Add("-Wall")	
end

-- OTAWA
settings.cc.flags:Add("`/home/jruiz/Documents/otawa/bin/otawa-config --cflags`")
settings.link.flags:Add("`/home/jruiz/Documents/otawa/bin/otawa-config --libs --rpath`")

-- CVC4
settings.cc.includes:Add("/home/jruiz/.local/include/")
settings.cc.flags:Add("-Wno-deprecated")
settings.link.libpath:Add("/home/jruiz/.local/lib")
settings.link.flags:Add("-lcvc4")

-- -lcvc4 -L/home/jruiz/.local/lib -I/home/jruiz/.local/include/ -Wno-deprecated

function Intermediate_Output(settings, input)
	return "objs/" .. string.sub(PathBase(input), string.len("src/")+1) .. settings.config_ext
end

-- Sends all objs to an objs/ directory
settings.cc.Output = Intermediate_Output

source = Collect("src/*.cpp")
objects = Compile(settings, source)
exe = Link(settings, "pathfinder", objects)
