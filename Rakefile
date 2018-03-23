require "bundler/gem_tasks"
require "rspec/core/rake_task"
require "rake/extensiontask"
require "yard"

RSpec::Core::RakeTask.new(:spec)

task :default => [:compile, :spec]

task :build => :compile

Rake::ExtensionTask.new("brotli") do |ext|
  ext.lib_dir = "lib/brotli"
end

YARD::Rake::YardocTask.new do |t|
  t.options = ["--no-progress", "--output-dir", "docs"]
end
