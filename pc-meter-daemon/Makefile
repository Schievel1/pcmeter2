##
# pc-meterd
#
# @file
# @version 0.1

PKGNAME ?= pc-meterd
PREFIX ?= /usr

SYSTEMD ?= true
OPENRC ?= false

SHARE_DIR = $(DESTDIR)$(PREFIX)/share
LIB_DIR = $(DESTDIR)$(PREFIX)/lib
BIN_DIR = $(DESTDIR)$(PREFIX)/bin
MAN_DIR = $(SHARE_DIR)/man

.PHONY: build install uninstall clean help

build:
	cargo build --release

install:
	@install -Dm744 -t "$(BIN_DIR)/" target/release/pc-meterd;
	@# Systemd init system
	@if test "$(SYSTEMD)" = true; then \
		echo "Installing systemd .service file"; \
		install -Dm644 -t "$(LIB_DIR)/systemd/system/" pc-meterd.service; \
	 fi
	@# OpenRC init system
	@if test "$(OPENRC)" = true; then \
		echo "Installing openRC init.d & conf.d file"; \
		install -Dm744 pc-meterd.initd "$(DESTDIR)/etc/init.d/pc-meterd"; \
		install -Dm644 pc-meterd.confd "$(DESTDIR)/etc/conf.d/pc-meterd"; \
	 fi

uninstall:
	@rm -f "$(LIB_DIR)/systemd/system/pc-meterd.service"
	@rm -f "$(BIN_DIR)/pc-meterd;"
	@rm -f "$(DESTDIR)/etc/init.d/pc-meterd;"
	@rm -f "$(DESTDIR)/etc/conf.d/pc-meterd;"

# end
