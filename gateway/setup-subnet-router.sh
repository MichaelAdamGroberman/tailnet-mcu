#!/usr/bin/env bash
# Turn ANY existing Tailscale Linux node into a WireGuard subnet router
# for a tailnet-mcu device. Idempotent. Run with sudo on the gateway node.
set -euo pipefail

WG_IF="${WG_IF:-wg0}"
WG_PORT="${WG_PORT:-51820}"
WG_SUBNET="${WG_SUBNET:-10.20.30.0/24}"
GW_ADDR="${GW_ADDR:-10.20.30.1}"
DEV_ADDR="${DEV_ADDR:-10.20.30.5}"
TS_IF="${TS_IF:-tailscale0}"

command -v tailscale >/dev/null || { echo "tailscale not installed"; exit 1; }
tailscale status >/dev/null   || { echo "this node is not on a tailnet (run 'tailscale up')"; exit 1; }
command -v wg >/dev/null      || { echo "install wireguard-tools first"; exit 1; }

umask 077
mkdir -p /etc/wireguard
[ -f /etc/wireguard/gw_private.key ] || wg genkey > /etc/wireguard/gw_private.key
gw_priv="$(cat /etc/wireguard/gw_private.key)"
gw_pub="$(printf '%s' "$gw_priv" | wg pubkey)"

# Device keypair (generated here for convenience; move to the device, then
# you may delete dev_private.key from the gateway).
[ -f /etc/wireguard/dev_private.key ] || wg genkey > /etc/wireguard/dev_private.key
dev_priv="$(cat /etc/wireguard/dev_private.key)"
dev_pub="$(printf '%s' "$dev_priv" | wg pubkey)"

cat > "/etc/wireguard/${WG_IF}.conf" <<EOF
[Interface]
Address = ${GW_ADDR}/24
ListenPort = ${WG_PORT}
PrivateKey = ${gw_priv}

[Peer]
# tailnet-mcu device — AllowedIPs scoped to its /32 (anti-spoof)
PublicKey = ${dev_pub}
AllowedIPs = ${DEV_ADDR}/32
EOF

# Forwarding + MASQUERADE from the WG subnet onto the tailnet.
sysctl -w net.ipv4.ip_forward=1
echo 'net.ipv4.ip_forward=1' > /etc/sysctl.d/99-tailnet-mcu.conf
if ! iptables -t nat -C POSTROUTING -s "${WG_SUBNET}" -o "${TS_IF}" -j MASQUERADE 2>/dev/null; then
  iptables -t nat -A POSTROUTING -s "${WG_SUBNET}" -o "${TS_IF}" -j MASQUERADE
fi

systemctl enable --now "wg-quick@${WG_IF}"
tailscale up --advertise-routes="${WG_SUBNET}" --reset

echo "=================================================================="
echo "Gateway public key : ${gw_pub}"
echo "Approve the route ${WG_SUBNET} in the Tailscale admin console."
echo
echo "Paste this into the device's secrets.h (set Endpoint to THIS node's"
echo "reachable underlay address — LAN IP on-LAN, or public IP if roaming):"
echo "------------------------------------------------------------------"
cat <<EOF
[Interface]
PrivateKey = ${dev_priv}
Address    = ${DEV_ADDR}/24
[Peer]
PublicKey  = ${gw_pub}
Endpoint   = <THIS_NODE_REACHABLE_ADDR>:${WG_PORT}
AllowedIPs = 0.0.0.0/0
EOF
echo "------------------------------------------------------------------"
echo "Then delete /etc/wireguard/dev_private.key from this gateway."
