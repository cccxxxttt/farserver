#!/bin/sh

install_path="/etc/"
install_file="/etc/rc.local"
bootsh="farserver-boot.sh"
path=`pwd`

if [ ! -d "$install_path" ]; then
	mkdir -p $install_path
fi

if [ ! -f "$install_file" ]; then
	touch $install_file
fi

# del farserver autoboot
sed -i '/'$bootsh' start/d' $install_file

# add farserver autoboot
echo $path"/"$bootsh" start" >> $install_file

# boot the farserver
$path"/"$bootsh start
