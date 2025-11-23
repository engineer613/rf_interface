#!/bin/bash

echo "=== Testing RealFlight Connection ==="
echo ""

# Find Windows host IP
WINDOWS_IP=$(ip route show | grep -i default | awk '{ print $3}')
echo "Windows host IP: $WINDOWS_IP"
echo ""

# Test common RealFlight ports
echo "Testing common RealFlight ports on $WINDOWS_IP:"
echo ""

for port in 18083 18084 18085; do
    echo -n "Port $port: "
    timeout 2 bash -c "cat < /dev/null > /dev/tcp/$WINDOWS_IP/$port" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✓ OPEN"
    else
        echo "✗ CLOSED/REFUSED"
    fi
done

echo ""
echo "If all ports show CLOSED, please verify in RealFlight:"
echo "  1. Settings -> Physics -> 'Link' tab"
echo "  2. Ensure 'Enable RealFlight Link' is checked"
echo "  3. Note the port number shown"
echo "  4. Restart RealFlight if you just enabled it"
