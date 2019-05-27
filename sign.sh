#!/bin/bash
# script for signing a Windows executable
# requires osslsigncode to be installed, and that
# you have a signing certificate in
#   $CODE_SIGN_BASE.spc
#
# Some helpful links:
# https://blog.didierstevens.com/2008/12/30/howto-make-your-own-cert-with-openssl/
# https://quotidian-ennui.github.io/blog/2013/06/07/signing-windows-installers-on-linux/
#
# valid timestamp servers:
# http://timestamp.verisign.com/scripts/timstamp.dll
# http://timestamp.comodoca.com/authenticode
#
code=$1
mv $code ./unsigned_binary
osslsigncode -spc $CODE_SIGN_BASE.spc -key $CODE_SIGN_BASE.key -t http://timestamp.verisign.com/scripts/timstamp.dll -in ./unsigned_binary -out $code
