# frozen_string_literal: true

$LOAD_PATH.unshift File.expand_path("../lib", __dir__)
require "brotli"

require "test-unit"
require "test/unit/rr"
require "rantly/testunit_extensions"
require "stringio"
