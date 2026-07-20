# Trimpod build glue for the vendored SQLite amalgamation (public domain).
# See lib/sqlite/PROVENANCE.md.

SQLITELIB_DIR := $(ROOTDIR)/lib/sqlite
SQLITELIB_SRC := $(call preprocess, $(SQLITELIB_DIR)/SOURCES)
SQLITELIB_OBJ := $(call c2obj, $(SQLITELIB_SRC))
SQLITELIB := $(BUILDDIR)/lib/libsqlite.a

OTHER_SRC += $(SQLITELIB_SRC)
INCLUDES += -I$(SQLITELIB_DIR)
CORE_LIBS += $(SQLITELIB)

# Build lean + single-threaded (Trimpod touches the DB from one thread only).
# -w: it is vendored verbatim, so we don't police its warning hygiene under
# Rockbox's -Wall -Wextra.
SQLITELIBFLAGS = $(CFLAGS) -w \
	-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION \
	-DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_DEPRECATED -DSQLITE_DQS=0

$(BUILDDIR)/lib/sqlite/%.o: $(SQLITELIB_DIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -c $< -o $@ \
	-I$(SQLITELIB_DIR) $(SQLITELIBFLAGS)

$(SQLITELIB): $(SQLITELIB_OBJ)
	$(SILENT)$(shell rm -f $@)
	$(call PRINTS,AR $(@F))$(AR) rcs $@ $^ >/dev/null
