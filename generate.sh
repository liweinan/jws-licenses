#!/usr/bin/env bash
set -x
rm -rf products
mkdir -p products
cp licenses/licenses.xml \
	./eap-licenses-generator/src/main/resources/template/
pushd eap-licenses-generator
mvn clean package
popd
mkdir -p products/licenses/
cp licenses/* products/licenses/
cp eap-licenses-generator/target/licenses/* \
    products/licenses/
