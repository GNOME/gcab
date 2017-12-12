all: test-none.cab test-mszip.cab test-signed.cab

test-none.cab: src/test.sh src/test.txt
	touch -t 201709150000 src/test.sh src/test.txt &
	gcab --create --nopath $@ src/test.sh src/test.txt

test-mszip.cab: src/test.sh src/test.txt
	gcab --create --nopath --zip $@ src/test.sh src/test.txt

privkey.pem:
	certtool --generate-privkey --outfile $@

test-signed.cab: privkey.pem test-none.cab
	osslsigncode sign -certs /etc/pki/fwupd/LVFS-CA.pem \
		-key privkey.pem \
		-n "Test Firmware" -i https://fwupd.org/ \
		-jp low \
		-in test-none.cab -out test-signed.cab
