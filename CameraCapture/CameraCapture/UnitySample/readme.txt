The UnitySample has .meta files for each plugin the solution will build. Be sure to build x86 and x64 before opening the Unity sample. Your final build should target Release configuration.

As well, binaries will be compiled to the solution's Build directory. The binaries are for Desktop/Win32(Unity Editor/Standalone) and Windows 10 Universal(UWP) apps. 

The post-build script will copy the binaries to the correct folders for the UnitySample(Assets/{SolutionName}/Plugins). If the .meta files are missing, update the import settings for each plugin for the correct cpu and platform. https://docs.unity3d.com/Manual/PluginInspector.html

For convience, the Unity Sample can be configured to debug plugin code. When Start Debugging(F5) is selected, this will launch the Unity Editor with the sample project.

To configure debugging:
    - Open the properties for the UnitySample project. 
    - Select the Debugging properties panel and modify the properties with the following values. 
        * adjust the Command properties to match your Unity install directory
            x64 Local Windows Debugger
            Command: $(ProgramW6432)\Unity 2017.4.8f1\Editor\Unity.exe
            CommandArguments: -projectPath $(ProjectDir)$(SolutionName)App
            WorkingDirectory: $(ProjectDir)$(SolutionName)App

            Win32 Local Windows Debugger
            Command: $(ProgramFiles)\Unity 2017.4.8f1\Editor\Unity.exe
            CommandArguments: -projectPath $(ProjectDir)$(SolutionName)App
            WorkingDirectory: $(ProjectDir)$(SolutionName)App
