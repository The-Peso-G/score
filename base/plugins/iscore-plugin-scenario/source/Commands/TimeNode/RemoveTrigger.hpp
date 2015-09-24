#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Commands/Constraint/SetRigidity.hpp"

#include "State/Expression.hpp"

class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
    class RemoveTrigger : public iscore::SerializableCommand
    {
	    ISCORE_COMMAND_DECL("ScenarioControl", "RemoveTrigger", "RemoveTrigger")
	public:
	    ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveTrigger)
	    RemoveTrigger(Path<TimeNodeModel>&& timeNodePath);
        ~RemoveTrigger();

	    virtual void undo() override;
	    virtual void redo() override;

	protected:
	    virtual void serializeImpl(QDataStream&) const override;
	    virtual void deserializeImpl(QDataStream&) override;

	private:
	    Path<TimeNodeModel> m_path;
        QVector<SetRigidity*> m_cmds;

    };

    }
}
