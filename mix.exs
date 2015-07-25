defmodule MessageQueue.Mixfile do
  use Mix.Project

  def project do
    [app: :message_queue,
     version: "0.0.1",
     elixir: "~> 1.0",
     compilers: [:make, :elixir, :app], # Add the make compiler
     build_embedded: Mix.env == :prod,
     start_permanent: Mix.env == :prod,
     deps: deps]
  end

  # Configuration for the OTP application
  #
  # Type `mix help compile.app` for more information
  def application do
    [applications: [:logger]]
  end

  # Dependencies can be Hex packages:
  #
  #   {:mydep, "~> 0.3.0"}
  #
  # Or git/path repositories:
  #
  #   {:mydep, git: "https://github.com/elixir-lang/mydep.git", tag: "0.1.0"}
  #
  # Type `mix help deps` for more examples and options
  defp deps do
    []
  end
end

###################
# Make file Tasks #
###################

defmodule Mix.Tasks.Compile.Make do
  use Mix.Task
  
  @shortdoc "Compiles helper in c_src"
  
  def run(_) do
    {result, _error_code} = System.cmd("make", [], stderr_to_stdout: true)
    Mix.shell.info result
    :ok
  end

  def clean() do
    {result, _error_code} = System.cmd("make", ['clean'], stderr_to_stdout: true)
    Mix.shell.info result
    :ok
  end   
end
