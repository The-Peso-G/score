#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>
#include <QAction>

class ScenarioCommand : public iscore::PluginControlInterface
{
		Q_OBJECT
	public:
		ScenarioCommand();
		virtual void populateMenus(iscore::MenubarManager*) override;
		virtual void populateToolbars() override;
		virtual void setPresenter(iscore::Presenter*) override;

        void emitCreateTimeEvent(QPointF pos);

	signals:
		void incrementProcesses();
		void decrementProcesses();
        void createTimeEvent(QPointF pos);

	private slots:
        void on_createTimeEvent(QPointF position);

	private:
		QAction* m_action_Scenarioigate;
		iscore::Presenter* m_presenter{};
};
