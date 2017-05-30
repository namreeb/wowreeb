# wowreeb
An intelligent, versatile, and multi-versioned World of Warcraft launcher

This application was made to have a reliable and efficient way to launch different versions of World of Warcraft without having to manually edit files.  It is configured in XML, and is lightweight enough to be loaded by Windows upon login.  It will create a system tray icon which will display a menu showing the different options defined in the configuration file.  An example configuration file is included.

## Technical Information

The launcher app is written in C# and includes a helper library written in C++.  The C++ DLL depends on hadesmem (https://github.com/RaptorFactor/hadesmem).  The helper DLL serves two purposes.  It is loaded by the launcher to create the World of Warcraft process, injecting itself at the same time.  When the helper DLL is loaded by the newly launched World of Warcraft process, it will adjust the environment in the manner requested by the launcher.  This includes setting the name of the authentication server and optionally adjusting the graphics engine field-of-view (FoV) value.

## Security

Though the included helper DLL (wowreeb.dll) is injected into the World of Warcrat process, it will eject itself once its initialization is complete.  Therefore the DLL itself should not be detectable by Warden.

**HOWEVER** if your configuration file defines a change to the FoV value, this change **is detectable** by Warden.  No reasonable server developer would bother to detect this, but use at your own risk!

It is also theoretically possible that the loading and unloading of the helper DLL does leave some residual footprint even after it is unloaded, and that that footprint may be detectable by Warden.  Again, nothing about this launcher would qualify as a 'hack' or an 'exploit', therefore it should be safe to use anywhere.

**TLDR**  Use at your own risk!