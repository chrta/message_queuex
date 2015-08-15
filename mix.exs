defmodule MessageQueue.Mixfile do
  use Mix.Project

	@github_link "https://github.com/chrta/message_queuex"

  def project do
    [app: :message_queue,
     version: "0.0.1",
     elixir: "~> 1.0",
     description: description,
     package: package,
		 source_url: @github_link,
     compilers: [:make, :elixir, :app], # Add the make compiler
     build_embedded: Mix.env == :prod,
     start_permanent: Mix.env == :prod,
     deps: deps,
		 test_coverage: [tool: Coverex.Task, coveralls: true]
		]
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
    [{:earmark, "~> 0.1", only: :dev},
		 {:ex_doc, "~> 0.8", only: :dev},
		 {:coverex, "~> 1.4.1", only: :test}
		]
  end

  defp description do
    """
    Provides an interface to posix message queues.

    See 'man mq_overview' for a general documentation.
    """
  end

  defp package do
    [
      files: ["lib", "priv", "mix.exs", "README.md", "LICENSE", "c_src*", "config*", "priv*", "test*"],
      contributors: ["Christian Taedcke"],
      licenses: ["Apache 2.0"],
      links: %{"GitHub" => @github_link}]
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
