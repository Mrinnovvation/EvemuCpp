# ESP and the XML API

EVEmu ships two optional HTTP services alongside the game and image servers. Both
are **disabled by default** and are enabled per-server in `eve-server.xml`.

| Service | Default port | What it is |
|---|---|---|
| API | 26002 | The `api.eveonline.com`-style XML API, for third-party tools. |
| ESP | 50001 | *EVE Server Pages* — the GM and game-data web pages the client's right-click menu links to. |

---

## ESP (EVE Server Pages)

### Why port 50001 is not configurable in practice

The client builds the ESP address itself. `loginII.GetServerInfo()` returns

```python
espUrl = <the IP the client connected to> + ':50001'
```

and `menusvc.GetFromESP()` opens `http://<espUrl>/<page>` with
`blue.os.ShellExecute()`. So the page opens in the operator's ordinary browser,
not inside the client, and the port is baked into the client. `espServerPort`
exists so you can move the listener, but the client's GM menu entries will only
resolve if it stays on 50001.

### Enabling it

```xml
<net>
    <espServerEnabled>true</espServerEnabled>
    <espServer>127.0.0.1</espServer>
    <espServerPort>50001</espServerPort>
</net>
```

### Signing in

Because the pages open in a normal browser there is no EVE session to authorise
against, so ESP uses HTTP Basic authentication against the `account` table. The
account must:

* not be banned, and
* hold at least one role in `Acct::Role::VIEW` — that is `ADMIN`, `CONTENT`,
  `GML`, `GMH` or `QA`.

This is the same mask the client's own `spwriters.py` uses to decide whether to
show the ESP menus at all, so an account that sees the menu in-game can sign in.

Banning an account additionally requires `BANNING`, `GMH` or `ADMIN`.

### Pages

| URL | Reached from |
|---|---|
| `/` | Typed directly. Server summary and index. |
| `/gm/character.py` | Menu bar. Character search. |
| `/gm/character.py?action=MyCharacters` | Menu bar, and the client's GM menu. |
| `/gm/character.py?action=Character&characterID=N` | Client: *GM → Show in ESP*. |
| `/gm/users.py` | Menu bar. Account list with ban/unban. |
| `/gm/users.py?action=BanUserByCharacterID&characterID=N` | Client: *GM → Ban User (ESP)*. |
| `/gd/type.py?action=Type&typeID=N` | Client: NPC menu → *Overview*. |
| `/gd/type.py?action=TypeDogma&typeID=N` | Client: NPC menu → *Dogma Attributes*. |
| `/gd/npc.py?action=GetNPCInfo&shipID=N&solarSystemID=N` | Client: NPC menu → *Info*. |

The page names and their `action` values are fixed by the client. Renaming one
breaks the corresponding right-click entry.

### Operational notes

* **Do not expose port 50001 to the internet.** These pages are GM tooling. They
  are served over plain HTTP, so Basic credentials cross the wire unencrypted;
  keep the listener on a trusted network or behind a TLS-terminating proxy.
* The client's ban entry is an ordinary link, so a `GET` only renders a
  confirmation page. The ban is applied by the `POST` behind it.
* State-changing `POST`s are refused unless `Origin`/`Referer` matches `Host`.
  Browsers replay Basic credentials on cross-site form posts, so without this any
  web page could have auto-submitted a ban.
* A ban takes effect at the account's next login attempt. It does not disconnect
  an already-connected client.
* Every ban and every rejected sign-in is written to the server log.

---

## The XML API

### Enabling it

```xml
<net>
    <apiServerEnabled>true</apiServerEnabled>
    <apiServer>127.0.0.1</apiServer>
    <apiServerPort>26002</apiServerPort>
</net>
```

The API stores its key pairs in the `accountApi` table; run the migrations
(`./evedbtool up`) before enabling it.

### Getting a key

`userID` is the API-facing account identifier and is **not** the same as
`accountID`.

```
http://<host>:26002/account/APIKeyRequest.xml.aspx
    ?username=<account>&password=<password>&keytype=full&action=new
```

`keytype` is `full` or `limited`; `action` is `new` (generate/replace) or `get`
(return the existing key). The response carries the `userID` and the key.

### Implemented calls

| Call | Auth |
|---|---|
| `/server/ServerStatus.xml.aspx` | none |
| `/account/APIKeyRequest.xml.aspx` | username + password |
| `/account/Characters.xml.aspx` | userID + apiKey |
| `/account/AccountStatus.xml.aspx` | userID + apiKey |
| `/char/CharacterSheet.xml.aspx` | userID + apiKey, character must be on that account |
| `/char/SkillQueue.xml.aspx` | as above |
| `/char/SkillInTraining.xml.aspx` | as above |

The `corp`, `map` and `eve` service managers are not implemented and return a
`9999` error. `APIActiveObjectManager` (`/object/...`) is a stub: its header
declares a set of live-object calls, but only the dispatcher exists.

Some `CharacterSheet` sections are still placeholders rather than live data:
`attributeEnhancers` (implants), `certificates`, corporation roles, and the clone
fields. Skills, learning attributes, balance, corporation and bio data are real.

### Notes

* A key only grants access to characters on its own account; requesting another
  account's character returns error `209`.
* Both key types are currently accepted wherever a limited key is required, and a
  full key also satisfies a limited-key check.
