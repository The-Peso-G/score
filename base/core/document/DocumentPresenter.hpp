#pragma once
#include <memory>
#include <QObject>

#include <core/presenter/command/CommandQueue.hpp>

namespace iscore
{
	class DocumentModel;
	class DocumentView;
	class DocumentDelegatePresenterInterface;
	
	/**
	 * @brief The DocumentPresenter class holds the logic for the main document.
	 * 
	 * Its main use is to manage the command queue, since we use the Command pattern, 
	 * by taking the commands from the document view and applying them on the document model.
	 */
	class DocumentPresenter : public QObject
	{
			Q_OBJECT
		public:
			DocumentPresenter(QObject* parent, DocumentModel*, DocumentView*);
			CommandQueue* commandQueue() { return m_commandQueue.get(); }

			void newDocument();
			void reset();
			void setPresenter(DocumentDelegatePresenterInterface* pres);

		private slots:
			void applyCommand(Command*);

		private:
			std::unique_ptr<CommandQueue> m_commandQueue;
			DocumentDelegatePresenterInterface* m_presenter{};
	};
}
