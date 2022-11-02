defmodule ConescanDbTool.Repo do
  use Ecto.Repo, otp_app: :conescan_db_tool, adapter: Ecto.Adapters.SQLite3
end
