describe 'database' do
  before do
    `rm -rf ./data/db.bin`
  end

  def run_script(commands)
    raw_output = nil
    IO.popen("./duck_db", "r+") do |pipe|
      commands.each do |command|
        begin
          pipe.puts command
        rescue Errno::EPIPE
          break
        end
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end


  it 'insert test' do
    script = (1..5000).map do |i|
      "insert db #{i} user#{i} #{i*12} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result.last(2)).to match_array([
      ">insert",
      "> bye!",
    ])
  end
end