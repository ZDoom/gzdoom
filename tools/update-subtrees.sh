#!/bin/bash

export GITROOT="$(git rev-parse --show-toplevel)"

pull() {
	if [[ "${1}" == "${2}" || "${1}" == "all" ]]
	then
		echo "fetching ${2}"
		git -C $GITROOT subtree pull --prefix="${3}" "${4}" "${5}" --squash || exit
	else
		echo "${2} not specified, skipping"
	fi
}

pull "$1" 'zwidget' 'libraries/ZWidget' 'https://github.com/dpjudas/ZWidget' 'master'
pull "$1" 'zmusic'  'libraries/ZMusic'  'https://github.com/ZDoom/ZMusic'    'master'
