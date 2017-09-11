#!/usr/bin/env bash
set -x
dirs=("httpd-rhel" "httpd-sun" "httpd-win" \
    "jsvc-rhel" "jsvc-sun" "jsvc-win" \
    "openssl-rhel" "openssl-sun" "openssl-win")
rm -rf products
mkdir -p products
for dir in "${dirs[@]}"
do
cp licenses/${dir}/licenses.xml \
	./eap-licenses-generator/src/main/resources/template/
pushd eap-licenses-generator
mvn clean package
popd
mkdir -p products/${dir}/licenses/
cp licenses/${dir}/* products/${dir}/licenses/
cp eap-licenses-generator/target/licenses/* \
    products/${dir}/licenses/
done