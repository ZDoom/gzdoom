#!/bin/bash

pushd "$(git rev-parse --show-toplevel)" >/dev/null

pull() {
	if [[ "${1}" == "${2}" || "${1}" == "all" ]]
	then
		echo "fetching ${2}"
		git subtree pull --prefix="${3}" "https://github.com/${4}" "${5}" --squash || exit
	else
		echo "${2} not specified, skipping"
	fi
}

pull "$1" 'zwidget' 'libraries/ZWidget' 'dpjudas/ZWidget' 'master'
pull "$1" 'zmusic'  'libraries/ZMusic'  'ZDoom/ZMusic'    'master'
