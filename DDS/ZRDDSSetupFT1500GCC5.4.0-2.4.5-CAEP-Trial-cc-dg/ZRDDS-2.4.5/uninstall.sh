#!/bin/bash
product_version=2.4.5
dst_dir=/usr/ZRDDS/ZRDDS-$product_version
# remove soft link
ln_paths=('bin/ZRDDSGen/zrddsgen' 'bin/ZRDDSLauncher/ZRDDSLauncher')
for ele in ${ln_paths[@]}
	do
		cur_file="$dst_dir/$ele"
		if [ -f $cur_file ]; then
			cur_file_name=$(basename $cur_file)
			cur_link_path="/usr/local/sbin/$cur_file_name"
			# remove soft link if exist
			if [ -L $cur_link_path ]; then
				echo "remove soft link $cur_link_path"
				sudo rm -rf $cur_link_path
			fi
		fi
	done
# remove all content
sudo rm -rf $dst_dir
# remove /usr/ZRDDS if empty
if [ "`ls -A /usr/ZRDDS`" == "" ]; then
	echo "/usr/ZRDDS is empty, remove it."
	sudo rm -rf /usr/ZRDDS
fi
echo "UnInstall ZRDDS-$product_version from $dst_dir finished!"
