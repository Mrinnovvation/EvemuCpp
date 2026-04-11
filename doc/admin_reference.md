# Admin Reference

## Grant Admin Role to Account

Log in once to create the account row, then use the helper script:

```bash
./utils/grant-admin.sh your_username
```

Or run the raw command manually:

```bash
docker exec db mysql -u evemu -pevemu evemu -e "UPDATE account SET role = 2013265920 WHERE accountName = 'your_username';"
```

The value `2013265920` is the full admin role bitmask. Relog after running.

---

## Useful In-Game GM Commands (requires admin role)

| Command | Description |
|---------|-------------|
| `/giveallskills me` | Train all skills to level 5 on your character |
| `/giveskill me <typeID> <level>` | Train a specific skill to a specific level |
| `/spawn <typeID>` | Spawn an NPC by type ID |
| `/roid <typeID> <radius>` | Spawn an asteroid |
| `/online me` | Online all modules on your current ship |
| `/search <name>` | Search items by name (results sent via evemail) |
