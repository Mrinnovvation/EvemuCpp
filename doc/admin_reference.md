# Admin Reference

## Grant Admin Role to Account

Run against the database to give a character full GM/admin access.
Replace `your_username` with the account name used to log in.

```bash
docker exec -it evemu_crucible_claude-db-1 mysql -u eve -peve eve -e "UPDATE account SET role = 2013265920 WHERE accountName = 'your_username';"
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
