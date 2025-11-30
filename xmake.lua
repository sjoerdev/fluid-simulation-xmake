-- dependencies
add_requires("glm")
add_requires("glfw")
add_requires("glad")
add_requires("tbb")

-- update vscode intellisense
add_rules("plugin.compile_commands.autoupdate", { outputdir = ".vscode" })

-- project
target("project")

    -- settings
    set_kind("binary")
    set_languages("c++17")

    -- sources
    add_files("src/*.cpp")

    -- linking
    add_packages("glfw", "glad", "glm", "tbb")