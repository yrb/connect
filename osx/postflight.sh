#!/bin/bash
cat > /Library/LaunchAgents/com.nonolith.connect.agent.plist << EOT
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.nonolith.connect.agent</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Applications/nonolith-connect.app/Contents/MacOS/nonolith-connect</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>
EOT
su $USER -c "/Applications/nonolith-connect.app/Contents/MacOS/nonolith-connect&disown"
