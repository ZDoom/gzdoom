read -p "This tool adds in missing CMAKE stuff to your llvm-3.8 package. This requires super-user access. This tool is only meant for convenience and is possibly very dangerous. Rude things may tend to occur when using this tool. Are you sure you want to use this tool?" -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
	sudo mkdir -p /usr/lib/llvm-3.8/share/llvm
	sudo ln -s /usr/share/llvm-3.8/cmake /usr/lib/llvm-3.8/share/llvm/cmake
	sudo sed -i -e '/get_filename_component(LLVM_INSTALL_PREFIX/ {s|^|#|}' -e '/^# Compute the installation prefix/i set(LLVM_INSTALL_PREFIX "/usr/lib/llvm-3.8")' /usr/lib/llvm-3.8/share/llvm/cmake/LLVMConfig.cmake
	sudo sed -i '/_IMPORT_CHECK_TARGETS Polly/ {s|^|#|}' /usr/lib/llvm-3.8/share/llvm/cmake/LLVMExports-relwithdebinfo.cmake
	sudo sed -i '/_IMPORT_CHECK_TARGETS sancov/ {s|^|#|}' /usr/lib/llvm-3.8/share/llvm/cmake/LLVMExports-relwithdebinfo.cmake
	sudo ln -s /usr/lib/x86_64-linux-gnu/libLLVM-3.8.so.1 /usr/lib/llvm-3.8/lib/
fi

