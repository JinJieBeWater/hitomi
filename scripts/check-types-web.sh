#!/bin/sh

set -eu

cd "$(dirname "$0")/.."/apps/web

mkdir -p ../../output/tmp/npm-cache

PATH="$(zsh -lc 'printf %s "$PATH"')"
export PATH

node ./node_modules/nuxt/bin/nuxt.mjs prepare
tsc --noEmit -p .nuxt/tsconfig.server.json
npm exec --yes --cache ../../output/tmp/npm-cache --package vue-tsc -- vue-tsc --noEmit -p tsconfig.typecheck.json
