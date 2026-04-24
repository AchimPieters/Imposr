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
    set theChoice to button returned of (display dialog ¬
        "Welcome to the " & kProductName & " " & kVersion & " installer for Adobe Acrobat." & return & return & ¬
        "• Click  Install  to add the plug-in to Acrobat." & return & ¬
        "• Click  Uninstall  to remove a previous installation." ¬
        buttons {"Uninstall", "Cancel", "Install"} ¬
        default button "Install" cancel button "Cancel" ¬
        with title kProductName & " Plug-in Installer" ¬
        with icon note)

    if theChoice is "Install" then
        doInstall()
    else if theChoice is "Uninstall" then
        doUninstall()
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

    -- Auto-detect Acrobat.
    set pluginsDir to findAcrobatPluginsDir()

    if pluginsDir is "" then
        -- Let the user locate Acrobat manually.
        try
            set acrobatApp to POSIX path of (choose file ¬
                with prompt "Adobe Acrobat was not found automatically. Please locate Adobe Acrobat.app:" ¬
                default location (path to applications folder))
            set pluginsDir to acrobatApp & "Contents/Plug-ins"
        on error
            -- User cancelled browse.
            return
        end try
    end if

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
