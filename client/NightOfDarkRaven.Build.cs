#UEBuildModule
#Name = NightOfDarkRaven
#Type = Runtime
#Version = 1

PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" })

PrivateDependencyModuleNames.AddRange(new string[] { "Engine" })


# Add new source files
Add.Collections.Add(new string[] { "NRVLoginWidget.cpp", "NRVAuthComponent.cpp" })