/*
    MIT License

    Copyright (c) 2017 namreeb http://github.com/namreeb legal@namreeb.org

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Windows.Forms;
using System.Xml.Linq;
using wowreeb.Properties;

namespace wowreeb
{
    class WowreebContext : ApplicationContext
    {
        [DllImport("wowreeb.dll", CharSet = CharSet.Unicode)]
        private static extern uint Inject([MarshalAs(UnmanagedType.LPWStr)] string exe,
            [MarshalAs(UnmanagedType.LPWStr)] string dll, [MarshalAs(UnmanagedType.LPStr)] string authServer, float fov,
            [MarshalAs(UnmanagedType.LPWStr)] string domainDll);

        private struct VersionEntry
        {
            public string Path;
            public string AuthServer;
            public string SHA256;
            public float Fov;
            public string CLRDll;
        }

        private readonly Dictionary<string, VersionEntry> _versionEntries = new Dictionary<string, VersionEntry>();
        private readonly NotifyIcon _trayIcon;

        public WowreebContext()
        {
            if (!ParseConfig("config.xml"))
            {
                MessageBox.Show(@"config.xml load failed");
                Application.Exit();
            }

            var menuItems = new List<MenuItem>(_versionEntries.Count);
            menuItems.AddRange(_versionEntries.Select(entry => new MenuItem(entry.Key, (sender, args) => Click(entry.Key))));
            menuItems.Add(new MenuItem("-"));
            menuItems.Add(new MenuItem("Exit", Exit));

            _trayIcon = new NotifyIcon
            {
                Icon = Resources.WoW,
                ContextMenu = new ContextMenu(menuItems.ToArray()),
                Visible = true,
                Text = @"Wowreeb Launcher"
            };
        }

        private bool CheckExecutableIntegrity(string path, string expected)
        {
            using (var sha256 = SHA256.Create())
            {
                using (var stream = File.OpenRead(path))
                {
                    var checksum = BitConverter.ToString(sha256.ComputeHash(stream)).Replace("-", string.Empty).ToLower();

                    return checksum == expected.ToLower();
                }
            }
        }

        private void Click(string entry)
        {
            if (!_versionEntries.ContainsKey(entry))
                throw new Exception("Could not find entry " + entry);

            var sha256 = _versionEntries[entry].SHA256;
            var path = _versionEntries[entry].Path;

            if (sha256 != string.Empty && !CheckExecutableIntegrity(path, sha256))
            {
                MessageBox.Show($"File {path} failed security check!", @"SHA256 Failure!", MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            // TODO: Add x64 support
            Inject(_versionEntries[entry].Path, Directory.GetCurrentDirectory() + "\\wowreeb.dll",
                _versionEntries[entry].AuthServer, _versionEntries[entry].Fov, _versionEntries[entry].CLRDll);
        }

        private void Exit(object sender, EventArgs e)
        {
            _trayIcon.Visible = false;
            Application.Exit();
        }

        private bool ParseConfig(string filename)
        {
            var doc = XElement.Load(filename);

            foreach (var c in doc.Descendants())
            {
                switch (c.Name.ToString().ToLower())
                {
                    case "realm":
                    {
                        var name = string.Empty;
                        var ins = new VersionEntry {AuthServer = string.Empty, SHA256 = string.Empty};

                        foreach (var a in c.Attributes())
                        {
                            switch (a.Name.ToString().ToLower())
                            {
                                case "name":
                                    name = a.Value;
                                    break;

                                default:
                                    return false;
                            }
                        }

                        foreach (var e in c.Elements())
                        {
                            switch (e.Name.ToString().ToLower())
                            {
                                case "exe":
                                    foreach (var attr in e.Attributes())
                                    {
                                        switch (attr.Name.ToString().ToLower())
                                        {
                                            case "path":
                                                ins.Path = attr.Value;
                                                break;
                                            case "sha256":
                                                ins.SHA256 = attr.Value;
                                                break;
                                            default:
                                                return false;
                                        }
                                    }
                                    break;

                                case "authserver":
                                    foreach (var attr in e.Attributes())
                                    {
                                        switch (attr.Name.ToString().ToLower())
                                        {
                                            case "host":
                                                ins.AuthServer = attr.Value;
                                                break;
                                            default:
                                                return false;
                                        }
                                    }
                                    break;

                                case "fov":
                                    foreach (var attr in e.Attributes())
                                    {
                                        switch (attr.Name.ToString().ToLower())
                                        {
                                            case "value":
                                                if (!float.TryParse(e.FirstAttribute.Value, out ins.Fov))
                                                    throw new FormatException("Unable to parse FoV");
                                                break;
                                            default:
                                                return false;
                                        }
                                    }
                                    break;

                                case "clr":
                                    foreach (var attr in e.Attributes())
                                    {
                                        switch (attr.Name.ToString().ToLower())
                                        {
                                            case "path":
                                                ins.CLRDll = attr.Value;
                                                break;
                                            default:
                                                return false;
                                        }
                                    }
                                    break;
                                default:
                                    return false;
                            }
                        }

                        if (name == string.Empty)
                            return false;

                        _versionEntries[name] = ins;

                        break;
                    }
                }
            }
            return true;
        }
    }
}
