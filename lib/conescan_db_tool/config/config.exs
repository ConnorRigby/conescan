import Config

config :conescan_db_tool, ConescanDbTool.Repo, [
  database: "conescan.db"
]

config :conescan_db_tool, ecto_repos: [
  ConescanDbTool.Repo
]
