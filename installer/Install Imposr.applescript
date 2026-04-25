-- Install Imposr.applescript
-- Double-click in the DMG to install or remove the Imposr Acrobat plug-in.
-- The .api file is bundled inside this app at Contents/Resources/.

property kPluginFileName : "AcrobatImpositionPlugin.api"
property kProductName : "Imposr"
property kVersion : "0.1.0"
property kMenuPath : "Plug-ins > Imposr > Imposition control panel"
property kIdentifier : "com.imposr.acrobat.plugin"

-- Supported Acrobat app bundles, newest first.
property kAcrobatBundles : {¬
    "/Applications/Adobe Acrobat DC/Adobe Acrobat.app", ¬
    "/Applications/Adobe Acrobat/Adobe Acrobat.app", ¬
    "/Applications/Adobe Acrobat 2020/Adobe Acrobat.app", ¬
    "/Applications/Adobe Acrobat 2017/Adobe Acrobat.app"}

-- ── Entry point ───────────────────────────────────────────────────────────────
on run
    set welcomeText to "Welcome to the " & kProductName & " " & kVersion & " installer for Adobe Acrobat." & return & return & ¬
        "Please select an option and click Continue."
    set actionList to {"Install a " & kProductName & " plug-in", "Uninstall a " & kProductName & " plug-in", "Open an Acrobat plug-in folder in the Finder"}
    set selectedAction to choose from list actionList with title (kProductName & " Plug-in Installer") with prompt welcomeText default items {"Install a " & kProductName & " plug-in"} OK button name "Continue" cancel button name "Quit"
    if selectedAction is false then return
    set selectedActionText to item 1 of selectedAction

    if selectedActionText is "Install a " & kProductName & " plug-in" then
        doInstall()
    else if selectedActionText is "Uninstall a " & kProductName & " plug-in" then
        doUninstall()
    else if selectedActionText is "Open an Acrobat plug-in folder in the Finder" then
        doOpenPluginsFolder()
    end if
end run

-- ── Helpers ───────────────────────────────────────────────────────────────────
on shellTest(cmd)
    try
        set result to do shell script cmd
        return result is "yes"
    on error
        return false
    end try
end shellTest

on findAcrobatPluginsDir()
    repeat with bundlePath in kAcrobatBundles
        if shellTest("test -d " & quoted form of (bundlePath as string) & " && echo yes || echo no") then
            return (bundlePath as string) & "/Contents/Plug-ins"
        end if
    end repeat
    return ""
end findAcrobatPluginsDir

on findAcrobatBundlePath()
    repeat with bundlePath in kAcrobatBundles
        if shellTest("test -d " & quoted form of (bundlePath as string) & " && echo yes || echo no") then
            return bundlePath as string
        end if
    end repeat
    return ""
end findAcrobatBundlePath

on chooseAcrobatBundlePath(currentBundlePath)
    try
        set acrobatAppAlias to choose application with prompt "Select Adobe Acrobat.app:" default location (path to applications folder)
        set acrobatAppPath to POSIX path of acrobatAppAlias
        if acrobatAppPath ends with "/" then set acrobatAppPath to text 1 thru -2 of acrobatAppPath
        if acrobatAppPath does not end with ".app" then
            display dialog "Please select an Adobe Acrobat.app bundle." buttons {"OK"} default button "OK" with title kProductName & " Installer" with icon caution
            return currentBundlePath
        end if
        return acrobatAppPath
    on error
        return currentBundlePath
    end try
end chooseAcrobatBundlePath

on apiSourcePath()
    -- The .api is stored in this app bundle's Contents/Resources/.
    set appPath to POSIX path of (path to me)
    return appPath & "Contents/Resources/" & kPluginFileName
end apiSourcePath

-- ── Install ───────────────────────────────────────────────────────────────────
on doInstall()
    set apiSrc to apiSourcePath()
    if not shellTest("test -f " & quoted form of apiSrc & " && echo yes || echo no") then
        display dialog ¬
            "The plug-in file could not be found inside this installer." & return & return & ¬
            "Please re-download " & kProductName & " and try again." ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Installer" with icon stop
        return
    end if

    set acrobatBundlePath to findAcrobatBundlePath()
    repeat
        set installTargetText to "Not selected"
        if acrobatBundlePath is not "" then set installTargetText to acrobatBundlePath
        set installChoice to button returned of (display dialog ¬
            "Will install to this copy of Acrobat, or click Browse to choose another." & return & return & ¬
            installTargetText & return & return & ¬
            "Plug-in to install:" & return & ¬
            kPluginFileName ¬
            buttons {"Quit", "Browse…", "Install"} default button "Install" ¬
            with title kProductName & " Plug-in Installer" ¬
            with icon note)
        if installChoice is "Quit" then return
        if installChoice is "Browse…" then
            set acrobatBundlePath to chooseAcrobatBundlePath(acrobatBundlePath)
            if acrobatBundlePath is "" then return
        else if installChoice is "Install" then
            exit repeat
        end if
    end repeat

    if acrobatBundlePath is "" then
        display dialog "No Adobe Acrobat app selected." buttons {"OK"} default button "OK" with title kProductName & " Installer" with icon caution
        return
    end if
    set pluginsDir to acrobatBundlePath & "/Contents/Plug-ins"

    set apiDst to pluginsDir & "/" & kPluginFileName

    -- Copy with elevated privileges.
    try
        do shell script ¬
            "mkdir -p " & quoted form of pluginsDir & " && " & ¬
            "cp -f " & quoted form of apiSrc & " " & quoted form of apiDst & " && " & ¬
            "xattr -dr com.apple.quarantine " & quoted form of apiDst ¬
            with administrator privileges
    on error errMsg
        display dialog "Installation failed." & return & return & errMsg ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Installer" with icon stop
        return
    end try

    display dialog ¬
        kProductName & " has been installed successfully." & return & return & ¬
        "Restart Adobe Acrobat completely, then find the plug-in at:" & return & return & ¬
        "    " & kMenuPath ¬
        buttons {"OK"} default button "OK" ¬
        with title kProductName & " Installer" with icon note
end doInstall

on doOpenPluginsFolder()
    set pluginsDir to findAcrobatPluginsDir()
    if pluginsDir is "" then
        set acrobatBundlePath to chooseAcrobatBundlePath("")
        if acrobatBundlePath is "" then return
        set pluginsDir to acrobatBundlePath & "/Contents/Plug-ins"
    end if

    try
        do shell script "mkdir -p " & quoted form of pluginsDir
        do shell script "open " & quoted form of pluginsDir
        display dialog "Opened plug-in folder:" & return & pluginsDir ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Plug-in Installer" with icon note
    on error errMsg
        display dialog "Could not open plug-in folder." & return & return & errMsg ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Plug-in Installer" with icon stop
    end try
end doOpenPluginsFolder

-- ── Uninstall ─────────────────────────────────────────────────────────────────
on doUninstall()
    -- Check if anything is actually installed first.
    set foundPaths to {}
    repeat with bundlePath in kAcrobatBundles
        set apiPath to (bundlePath as string) & "/Contents/Plug-ins/" & kPluginFileName
        if shellTest("test -f " & quoted form of apiPath & " && echo yes || echo no") then
            set end of foundPaths to apiPath
        end if
    end repeat

    if (count of foundPaths) is 0 then
        display dialog "No installation of " & kProductName & " was found on this computer." ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Uninstaller" with icon caution
        return
    end if

    set confirmBtn to button returned of (display dialog ¬
        "This will remove " & kProductName & " from Adobe Acrobat." & return & return & ¬
        "Adobe Acrobat must be closed before uninstalling." & return & return & ¬
        "Continue?" ¬
        buttons {"Cancel", "Remove"} default button "Cancel" cancel button "Cancel" ¬
        with title kProductName & " Uninstaller" with icon caution)

    if confirmBtn is not "Remove" then return

    set removedCount to 0
    repeat with apiPath in foundPaths
        try
            do shell script "rm -f " & quoted form of (apiPath as string) ¬
                with administrator privileges
            set removedCount to removedCount + 1
        end try
    end repeat

    if removedCount > 0 then
        display dialog ¬
            kProductName & " has been removed." & return & return & ¬
            "Restart Adobe Acrobat to complete the uninstallation." ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Uninstaller" with icon note
    else
        display dialog "Removal failed. You may need to remove the plug-in manually." & return & return & ¬
            "Plug-in location:  <Acrobat.app>/Contents/Plug-ins/" & kPluginFileName ¬
            buttons {"OK"} default button "OK" ¬
            with title kProductName & " Uninstaller" with icon stop
    end if
end doUninstall
