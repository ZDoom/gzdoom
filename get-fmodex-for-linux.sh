#!/bin/sh

changelog=revision_4.44.txt

# get latest fmod ex version
wget http://www.fmod.org/files/$changelog

pointversion=$(grep -e "Stable branch update" $changelog | cut -d' ' -f2 | head -n1)
version=$(echo $pointversion | sed -e 's/\.//g')
rm -f $changelog

dirname=fmodapi${version}linux
fname=${dirname}.tar.gz

# download fmod ex
if [ ! -f $fname ] ; then
    wget "http://www.fmod.org/download/fmodex/api/Linux/$fname"
fi

# extract fmod ex
tar xvf $fname

if [ "$(uname -m)" = "x86_64" ]; then 
    X64=64
fi
fmodlib=$dirname/api/lib/libfmodex$X64-$pointversion.so

cp $fmodlib .

echo ""
echo ""
echo "Use the these paths for cmake:"
echo "FMOD_LIBRARY=$fmodlib"
echo "FMOD_INCLUDE_DIR=$dirname/api/inc"
