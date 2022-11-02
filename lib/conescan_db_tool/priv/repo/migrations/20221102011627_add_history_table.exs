defmodule ConescanDbTool.Repo.Migrations.AddHistoryTable do
  use Ecto.Migration

  def change do
    create table(:history) do
      add :path, :string, null: false
      add :type, :string, null: false
    end
  end
end
