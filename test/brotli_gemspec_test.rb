# frozen_string_literal: true

require "test_helper"

class BrotliGemspecTest < Test::Unit::TestCase
  test "does not package generated extension sources or dictionary blobs" do
    spec = Gem::Specification.load(File.expand_path("../brotli.gemspec", __dir__))

    assert_empty spec.files.grep(/\Aext\/brotli\/(?:common|dec|enc|include)\//)
    assert_empty spec.files.grep(/dictionary\.bin(?:\.br)?\z/)
  end
end
