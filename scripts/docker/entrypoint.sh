#!/usr/bin/env bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

set -e

if [[ -n "$GITLAB_CI" ]]; then
    exec "$@"
fi

if [[ -n "$DEBUG" ]]; then
    set -x
fi

if [[ -n "$TIMEZONE" ]]; then
    echo "$TIMEZONE" >/etc/timezone
    ln -sf /usr/share/zoneinfo/"$TIMEZONE" /etc/localtime
    dpkg-reconfigure -f noninteractive tzdata
fi

# Check if the group already exists
if getent group "$GROUP" >/dev/null 2>&1; then
    echo "Group $GROUP exists, skipping group creation..."
else
    groupadd -og "$GROUP_ID" "$GROUP"
fi

# Check if the user already exists
if getent passwd "$USER" >/dev/null 2>&1; then
    echo "User $USER exists, skipping user creation..."
else
    # Check if UID 1000 is occupied, if so, delete the user
    if getent passwd 1000 >/dev/null 2>&1; then
        username=$(getent passwd 1000 | cut -d: -f1)
        userdel -r $username
    fi

    # Find an available UID starting from USER_ID
    while getent passwd "$USER_ID" >/dev/null 2>&1; do
        USER_ID=$((USER_ID + 1))
    done

    useradd -M -u "$USER_ID" -g "$GROUP_ID" -d "/home/$USER" -s /bin/bash "$USER"
fi

# Check if the video group already exists
if getent group video >/dev/null 2>&1; then
    echo "Group video exists, skipping video group creation..."
else
    groupadd -og "$VIDEO_GROUP_ID" video
fi

gpasswd -a "$USER" video
usermod -aG sudo "$USER"
echo "$USER:0" | chpasswd

shopt -s dotglob
for x in /etc/skel/*; do
    target="/home/$USER/$(basename "$x")"
    if [[ ! -e "$target" ]]; then
        cp -a "$x" "$target"
        chown -R "$USER":"$GROUP" "$target"
    fi
done
shopt -u dotglob

if [[ -z "$SKIP_ADEINIT" ]]; then
    for x in /opt/*; do
        if [[ -x "$x/.adeinit" ]]; then
            echo "Initializing $x"
            sudo -Hu "$USER" -- bash -lc "$x/.adeinit"
            echo "Initializing $x done"
        fi
    done
fi

echo 'ADE startup completed.'
exec "$@"

