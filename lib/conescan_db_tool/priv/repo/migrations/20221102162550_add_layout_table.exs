defmodule ConescanDbTool.Repo.Migrations.AddLayoutTable do
  use Ecto.Migration

  def change do
    create table(:layout) do
      add :data, :binary, null: false
    end
  end
end
