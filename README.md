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

[sync]
; 1: push automatically on launch, without pressing anything.
;    Also read by the background service (see below).
autoPushOnLaunch=0
; Minimum hours between two automatic backups. App and background service
; share this limit, so they never back up the same data twice.
autoPushIntervalHours=24
; 1: back up every user profile registered on the console.
; 0: only the one named in defaultAccountName.
allAccounts=1
```

#### `[account]` behavior matrix

| Launch context | `useProfileSelector=1` (default) | `useProfileSelector=0` |
|---|---|---|
| Full application mode (forwarder / title takeover) | Use psel applet → fall back to `defaultAccountName` | Always use `defaultAccountName` |
| Applet mode (hbmenu via album applet) | Use `defaultAccountName` (psel is unavailable to library applets) | Use `defaultAccountName` |

If `defaultAccountName` is unset (or does not match any registered user) when the client needs it, uNSS prints an explanatory message and only the Exit option is available.

### Automatic backup

With `autoPushOnLaunch=1` the client starts pushing as soon as it opens — no
menu interaction. Two rules keep that from being wasteful or unsafe:

* **Only what changed.** The newest modification time inside each save is
  compared against the last upload (`sdmc:/uNSS/saves/.syncstate`). Unchanged
  titles are skipped before they are even archived.
* **Not while playing.** A running game keeps its save file open, so a backup
  taken at that moment can be inconsistent. The automatic push waits; the
  manual *Push to Server* button is never blocked.

The timestamp for `autoPushIntervalHours` is only written after a successful
run, so a failed backup is retried on the next launch instead of being
counted as done.

## Background service

An NRO only runs while it is open — start a game and it is gone. For backups
that happen without you, uNSS ships a sysmodule that boots with the console,
backs up, and exits.

Install it from inside the app: **Install background service**. It unpacks the
module to `atmosphere/contents/4200000000554E53/`, sets the boot2 flag and
takes effect after a **reboot**. **Remove background service** undoes it.
An already installed module is updated silently when the app carries a newer
one; only the first install is a deliberate button press, since it starts a
process at every boot.

The service reads the same `config.ini` and needs `remote.enabled=1` and
`sync.autoPushOnLaunch=1`. It has no user interface, so it cannot show a
profile selector: with `allAccounts=0` it depends entirely on
`defaultAccountName`, which is compared **case-sensitively**.

It writes to `sdmc:/uNSS/sysmodule.log`. Restoring is deliberately not part of
it — writing save data back should be a decision you watch happen, in the app.

> **Note on memory.** Sysmodules share a small pool, and it got tighter with
> firmware 20.0.0. `INNER_HEAP_SIZE` is set to a measured-safe 2 MiB. Raising
> it is not a local decision: at 6 MiB the HID system module could no longer
> start and the console ended up in a `2001-0132` boot loop, recoverable only
> by deleting the flag from the SD card on a PC.

## Server
### Prerequisite
Running server via Python interpreter requires some dependencies. Install dependencies first.
```bash
pip install -r requirements.txt
```

### Linux / macOS
Foreground mode

```bash
./run-linux.sh
```

or

```bash
python main.py --host 0.0.0.0 --port 8989
```

Background mode

```bash
setsid nohup ./run-linux.sh > server.log 2>&1 < /dev/null &
```

`nohup` alone is not enough: it only shields against `SIGHUP`. Closing the
terminal or pressing Ctrl-C sends `SIGINT` to the whole foreground process
group, and uvicorn shuts down cleanly on that — the log then shows a tidy
shutdown rather than a crash, which is easy to misread. `setsid` puts the
server in its own session, out of reach of both.

Run it from the `server/` directory: `metadata.sqlite` and `savedata/` are
resolved relative to the working directory.

### Windows
Just used prebuilt binary by PyInstaller

### Docker

```bash
docker compose -f docker-compose.yaml up -d
```

`compose.traefik.yml` is an alternative for setups that already run Traefik:
the container publishes no port of its own and is reached inside the docker
network, with TLS terminated by the proxy.

Access control there is basicAuth, not a login portal — the client does not
follow redirects, so anything redirect-based (authelia, OIDC) can never
complete. libcurl does send credentials taken straight from the URL, so
`serverUrl=https://user:password@host` works without any client change.

Generate the hash with an explicit cost; the `htpasswd` default of 5 is far
too low, and Traefik's basicAuth supports only MD5, SHA1 and bcrypt (no
argon2), so cost and password length carry the security:

```bash
htpasswd -nbBC 12 switch '<password>'
```

Images are published to `ghcr.io/<owner>/unss-server` by
`.github/workflows/publish-server-image.yml` on pushes to `main` and on `v*`
tags, for amd64 and arm64.
