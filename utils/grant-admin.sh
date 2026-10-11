#!/bin/bash
# Grant full GM/admin role to an EVEmu account.
# Usage: ./utils/grant-admin.sh <accountName>
# Run after logging in at least once so the account row exists.

if [ -z "$1" ]; then
    echo "Usage: $0 <accountName>"
    exit 1
fi

docker exec db mysql -u evemu -pevemu evemu \
    -e "UPDATE account SET role = 2013265920 WHERE accountName = '$1';"

echo "Admin role granted to '$1'. Relog to take effect."
