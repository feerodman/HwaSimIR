#!/bin/bash
if [ `id -u` -ne 0 ]; then
	echo "please run install.sh with root or sudo privilege."
	exit
fi
product_version=2.4.5
src_dir=ZRDDS-$product_version
dst_dir=/usr/ZRDDS/ZRDDS-$product_version
mkdir -p $dst_dir
cp -r $src_dir /usr/ZRDDS 
echo "export ZRDDS_HOME=$dst_dir" >> /etc/profile
while true
do
	read -p"Do you wish to echo \"export LD_LIBRARY_PATH=$dst_dir/lib:\$LD_LIBRARY_PATH\" >> /etc/profile?" yn
	case $yn in
		[Yy]* ) echo "export LD_LIBRARY_PATH=$dst_dir/lib:$LD_LIBRARY_PATH" >> /etc/profile; break;;
		[Nn]* ) break;;
		* ) echo "Please answer y or n.";;
	esac
done
source /etc/profile
# files x permission needed
bin_paths=('uninstall.sh' 'bin/ZRDDSLicenseUtil/LicenseInfoUtil' 'bin/ZRDDSGen/zrddsgen' 'bin/ZRDDSGen/zrddsgenUI' 'bin/ZRDDSLauncher/ZRDDSLauncher' 'bin/ZRDDSJudger/ZRDDSJudger' 'bin/ZRDDSJudger/start.sh' 'bin/ZRDDSDoctor/start.sh' 'bin/ZRDDSDoctor/ZRDDSDoctor' 'bin/ZRDDSDoctor/ZRDDSDoctorGUI' 'bin/ZRDDSPerf/zrddsperf')
for ele in ${bin_paths[@]}
	do
		cur_file="$dst_dir/$ele"
		if [ -f $cur_file ]; then
			# add x permission
			chmod a+x $cur_file
		fi
	done
# files need create soft link
ln_paths=('bin/ZRDDSGen/zrddsgen' 'bin/ZRDDSLauncher/ZRDDSLauncher')
for ele in ${ln_paths[@]}
	do
		cur_file="$dst_dir/$ele"
		if [ -f $cur_file ]; then
			cur_file_name=$(basename $cur_file)
			cur_link_path="/usr/local/sbin/$cur_file_name"
			# add soft link in sbin, so user can lanucher in any location
			if [ -L $cur_link_path ]; then
				echo "remove old soft link $cur_link_path"
				rm -rf $cur_link_path
			fi
			echo "create $cur_link_path to $cur_file"
			ln -s $cur_file /usr/local/sbin
		fi
	done

# create doc/index.html link
if [ -L $dst_dir/doc/index_c.html ]; then
	echo "remove old soft link $dst_dir/doc/index_c.html"
	rm -rf $dst_dir/doc/index_c.html
	ln -s $dst_dir/doc/cdoc/html/index.html $dst_dir/doc/index_c.html
fi
if [ -L $dst_dir/doc/index_cpp.html ]; then
	echo "remove old soft link $dst_dir/doc/index_cpp.html"
	rm -rf $dst_dir/doc/index_cpp.html
	ln -s $dst_dir/doc/cppdoc/html/index.html $dst_dir/doc/index_cpp.html
fi

echo "Install ZRDDS-$product_version to $dst_dir finished!"
# if launcher exist, run it
if [ -f "$dst_dir/bin/ZRDDSLauncher/ZRDDSLauncher" ]; then
	echo "run ZRDDSLauncher..."
	ZRDDSLauncher 1&
fi
# if release notes exist, open it
if [ -f "$dst_dir/doc/版本发布记录.html" ]; then
	echo "open releaseNote.html..."
	#firefox "$dst_dir/doc/版本发布记录.html"&
fi
