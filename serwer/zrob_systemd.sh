#!/bin/bash

SERVICE_NAME="stunp2p2"
BINARY_NAME="egzek"
EXEC_PATH="$(pwd)/$BINARY_NAME"
USER_NAME=$(whoami)

# Sprawdzenie czy jest program
if [ ! -f "$EXEC_PATH" ]; then
    echo "Błąd: najpierw skompiluj serwer"
    exit 1
fi

#to zrobi plik stunp2p2.service w tej sciezce co nizej zeby moglo to systemd obslugiwac
cat <<EOF | sudo tee /etc/systemd/system/$SERVICE_NAME.service
[Unit]
Description=p2p2 serwer :)
After=network.target

[Service]
WorkingDirectory=$(pwd)
ExecStart=$EXEC_PATH
Restart=always
RestartSec=5
User=$USER_NAME
StandardOutput=append:/var/log/$SERVICE_NAME.log
StandardError=inherit

[Install]
WantedBy=multi-user.target
EOF

echo "Przeładowanie i uruchomienie"
sudo systemctl daemon-reload
sudo systemctl enable $SERVICE_NAME
sudo systemctl restart $SERVICE_NAME

echo "Koniec!"
echo "status: systemctl status $SERVICE_NAME"
echo "logi: journalctl -u $SERVICE_NAME -f"