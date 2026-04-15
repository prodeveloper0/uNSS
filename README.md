# uNX Save Sync
uNX Save Sync (aka. uNSS) is a Nintendo Switch application that allows synchronization of save data between multiple devices through a central remote server.

central remote server manages save data with internal revision IDs for each user and title.

# Usages
## Client
![uNSS Client Screen](resources/clientscreen.jpg)
* How save data to push to remote? __JUST PRESS PUSH BUTTON__.
* How save data to pull from remote? __JUST PRESS PULL BUTTON__.

### Configuration
To use remote server synchronization, you must configure settings first. and uNSS client reads settings from `sdmc:/uNSS/config.ini`

```ini
[remote]
enabled=1
serverUrl=http://your.hostname.com:8989

[account]
; Nickname of the Switch user profile to operate on.
; Must exactly match the nickname shown in the system's "My Page".
defaultAccountName=MyNickname
; 1 (default): use psel applet (profile selector) first,
;              fall back to defaultAccountName only when that fails
;              (this is always the case in applet mode, since a
;              library applet cannot launch psel).
; 0          : skip psel entirely and always resolve the account
;              from defaultAccountName.
useProfileSelector=1

[title]
; Which titles to include when pushing save data (archiving).
; "created" (default): only titles that already have save data on this console.
; "all"               : every installed title, even if it has never been saved.
archiveBy=created

; Which titles to include when pulling save data (restoring).
; "all" (default)     : every installed title, even if it has never been saved.
;                       uNSS will automatically create save data for titles
;                       that don't have any yet.
; "created"           : only titles that already have save data on this console.
restoreBy=all

; Exclude specific titles by title ID (hex, comma-separated).
; excludedTitleIds=0100000000010000,010000000000100B

; Exclude specific titles by name (separated by "||").
; excludedTitleNames=uNSS||DBI
```

#### `[account]` behavior matrix

| Launch context | `useProfileSelector=1` (default) | `useProfileSelector=0` |
|---|---|---|
| Full application mode (forwarder / title takeover) | Use psel applet → fall back to `defaultAccountName` | Always use `defaultAccountName` |
| Applet mode (hbmenu via album applet) | Use `defaultAccountName` (psel is unavailable to library applets) | Use `defaultAccountName` |

If `defaultAccountName` is unset (or does not match any registered user) when the client needs it, uNSS prints an explanatory message and only the Exit option is available.

## Server
### Prerequisite
Running server via Python interpreter requires some dependencies. Install dependencies first.
```bash
pip install -r requirements.txt
```

### Linux / macOS
Background mode
```bash
nohup run-linux.sh
```

Foreground mode

```bash
./run-linux.sh
```

or

```bash
python main.py --host 0.0.0.0 --port 8989
```

### Windows
Just used prebuilt binary by PyInstaller
