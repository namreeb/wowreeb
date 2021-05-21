# wowreeb
An intelligent, versatile, and multi-versioned World of Warcraft launcher

This application was made to have a reliable and efficient way to launch different versions of World of Warcraft without having to manually edit files.  It is configured in XML, and is lightweight enough to be loaded by Windows upon login.  It will create a system tray icon which will display a menu showing the different options defined in the configuration file.  An example configuration file is included.

## Configuration

Included in the source as well as binary tarballs is an `example_config.xml` file.  Use this as a guide for configuring wowreeb for your system.  **It must be renamed to `config.xml` in order for wowreeb to find it.**  Explanations for the various settings are included in comments in the example configuration file.

## New In Version 3.0

### Automatic Login! ###

You can now store encrypted credentials in your configuration file.  When present, these credentials will be used to perform an automatic login when the client launches.  Note that if you logout and return to the login screen, you will not be automatically logged in again.  This is because by that point we have removed ourselves from the process.

### Credential Security ###

To leverage automatic login functionality, you must first set a key.  This key will be used as a master password to encrypt credentials in the configuration file.  There is a new popup menu item to encrypt a password.  If you have not yet provided a key, you will first be prompted to specify one.  Once a key has been chosen, you can use this menu item to encrypt arbitrary passwords for inclusion in the configuration file.  For a new user the process is essentially this:

1.  Run wowreeb
2.  Click on wowreeb icon in the notification area
3.  Click on "Encrypt Password"
4.  Specify a key that will be used as a master password for all other credentials
5.  Specify a password to encrypt with the key
6.  The encrypted password will be copied to the clipboard for convenient pasting into your configuration file
7.  Adjust the configuration file to include your new credentials (see `example_config.xml` for an example of what this looks like)
8.  Exit and re-run wowreeb to reload the new configuration file

When wowreeb launches and observes credentials in the configuration file, you must first authenticate with your key before wowreeb will load.

### Privacy ###

Each time I post an update, some chalkeating carebear will question my motivation in releasing an application like this.  My only motivation is the fact that I created this for myself and released it because I suspect it will be useful to others.  If you don't trust it, don't use it, and I will try not to lose any sleep.

"namreeb, how do I know that you are not stealing my passwords?"

Because it's open source, because I don't care about your passwords, and because bite me.

### Environment Variables ###

There are two optional environment variables which can be set when launching wowreeb:

* `WOWREEB_ENTRY` specifies the name of a configuration file entry to launch immediately
* `WOWREEB_KEY` specifies the key used for credentials encrypted in the configuration file.  When this is present the user is not prompted for the key when wowreeb loads.  There are obvious security concerns here, but if someone has access to your computer to read environment variables, they probably have access to intercept/record your credentials anyway.

## Technical Information

This application is written in C++.  It includes a 32 bit and 64 bit executable and DLL.  The project depends on hadesmem (https://github.com/namreeb/hadesmem).  When the helper DLL is loaded by a newly launched World of Warcraft process, it will adjust the environment in the manner requested by the launcher.  This includes setting the name of the authentication server and optionally adjusting the graphics engine field-of-view (FoV) value.

## Support ##

I have included an example configuration file and described in this document everything needed to get the application running.  If you are having problems it is probably because you did not configure things properly.  This application is designed to be lightweight and easily maintained.  User experience and error feedback are not high priorities.  If you feel that you have discovered a bug, please feel free to open an issue on the tracker here.  Vague, ambiguous or otherwise unhelpful issues will be closed.

## Account Security

Though the included helper DLL (wowreeb32.dll or wowreeb64.dll) is injected into the World of Warcraft process, it will eject itself once its initialization is complete.  Therefore the DLL itself should not be detectable by Warden.

**HOWEVER** if your configuration file defines a change to the FoV value, this change **is detectable** by Warden.  No reasonable server developer would bother to detect this, but use at your own risk!

It is also theoretically possible that the loading and unloading of the helper DLL does leave some residual footprint even after it is unloaded, and that that footprint may be detectable by Warden.  Again, nothing about this launcher would qualify as a 'hack' or an 'exploit', therefore it should be safe to use anywhere.

## TLDR ##

**Use at your own risk!**
